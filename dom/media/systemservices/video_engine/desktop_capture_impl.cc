/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_capture/video_capture_impl.h"

#include <stdlib.h>
#include <string>

#include "common_types.h"
#include "api/video/i420_buffer.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "libyuv.h"  // NOLINT
#include "modules/include/module_common_types.h"
#include "modules/video_capture/video_capture_config.h"
#include "system_wrappers/include/clock.h"
#include "rtc_base/logging.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_base/trace_event.h"
#include "video_engine/desktop_capture_impl.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_device_info.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/video_capture/video_capture.h"

#if defined(_WIN32)
#  include "platform_uithread.h"
#else
#  include "rtc_base/platform_thread.h"
#endif

namespace webrtc {

ScreenDeviceInfoImpl::ScreenDeviceInfoImpl(const int32_t id) : _id(id) {}

ScreenDeviceInfoImpl::~ScreenDeviceInfoImpl(void) {}

int32_t ScreenDeviceInfoImpl::Init() {
  desktop_device_info_ =
      std::unique_ptr<DesktopDeviceInfo>(DesktopDeviceInfoImpl::Create());
  return 0;
}

int32_t ScreenDeviceInfoImpl::Refresh() {
  desktop_device_info_->Refresh();
  return 0;
}

uint32_t ScreenDeviceInfoImpl::NumberOfDevices() {
  return desktop_device_info_->getDisplayDeviceCount();
}

int32_t ScreenDeviceInfoImpl::GetDeviceName(
    uint32_t deviceNumber, char* deviceNameUTF8, uint32_t deviceNameUTF8Length,
    char* deviceUniqueIdUTF8, uint32_t deviceUniqueIdUTF8Length,
    char* productUniqueIdUTF8, uint32_t productUniqueIdUTF8Length, pid_t* pid) {
  DesktopDisplayDevice desktopDisplayDevice;

  // always initialize output
  if (deviceNameUTF8 && deviceNameUTF8Length > 0) {
    memset(deviceNameUTF8, 0, deviceNameUTF8Length);
  }

  if (deviceUniqueIdUTF8 && deviceUniqueIdUTF8Length > 0) {
    memset(deviceUniqueIdUTF8, 0, deviceUniqueIdUTF8Length);
  }
  if (productUniqueIdUTF8 && productUniqueIdUTF8Length > 0) {
    memset(productUniqueIdUTF8, 0, productUniqueIdUTF8Length);
  }

  if (desktop_device_info_->getDesktopDisplayDeviceInfo(
          deviceNumber, desktopDisplayDevice) == 0) {
    size_t len;

    const char* deviceName = desktopDisplayDevice.getDeviceName();
    len = deviceName ? strlen(deviceName) : 0;
    if (len && deviceNameUTF8 && len <= deviceNameUTF8Length) {
      memcpy(deviceNameUTF8, deviceName, len);
    }

    const char* deviceUniqueId = desktopDisplayDevice.getUniqueIdName();
    len = deviceUniqueId ? strlen(deviceUniqueId) : 0;
    if (len && deviceUniqueIdUTF8 && len <= deviceUniqueIdUTF8Length) {
      memcpy(deviceUniqueIdUTF8, deviceUniqueId, len);
    }
  }

  return 0;
}

int32_t ScreenDeviceInfoImpl::DisplayCaptureSettingsDialogBox(
    const char* deviceUniqueIdUTF8, const char* dialogTitleUTF8,
    void* parentWindow, uint32_t positionX, uint32_t positionY) {
  // no device properties to change
  return 0;
}

int32_t ScreenDeviceInfoImpl::NumberOfCapabilities(
    const char* deviceUniqueIdUTF8) {
  return 0;
}

int32_t ScreenDeviceInfoImpl::GetCapability(
    const char* deviceUniqueIdUTF8, const uint32_t deviceCapabilityNumber,
    VideoCaptureCapability& capability) {
  return 0;
}

int32_t ScreenDeviceInfoImpl::GetBestMatchedCapability(
    const char* deviceUniqueIdUTF8, const VideoCaptureCapability& requested,
    VideoCaptureCapability& resulting) {
  return 0;
}

int32_t ScreenDeviceInfoImpl::GetOrientation(const char* deviceUniqueIdUTF8,
                                             VideoRotation& orientation) {
  return 0;
}

AppDeviceInfoImpl::AppDeviceInfoImpl(const int32_t id) {}

AppDeviceInfoImpl::~AppDeviceInfoImpl(void) {}

int32_t AppDeviceInfoImpl::Init() {
  desktop_device_info_ =
      std::unique_ptr<DesktopDeviceInfo>(DesktopDeviceInfoImpl::Create());
  return 0;
}

int32_t AppDeviceInfoImpl::Refresh() {
  desktop_device_info_->Refresh();
  return 0;
}

uint32_t AppDeviceInfoImpl::NumberOfDevices() {
  return desktop_device_info_->getApplicationCount();
}

int32_t AppDeviceInfoImpl::GetDeviceName(
    uint32_t deviceNumber, char* deviceNameUTF8, uint32_t deviceNameUTF8Length,
    char* deviceUniqueIdUTF8, uint32_t deviceUniqueIdUTF8Length,
    char* productUniqueIdUTF8, uint32_t productUniqueIdUTF8Length, pid_t* pid) {
  DesktopApplication desktopApplication;

  // always initialize output
  if (deviceNameUTF8Length && deviceNameUTF8Length > 0) {
    memset(deviceNameUTF8, 0, deviceNameUTF8Length);
  }
  if (deviceUniqueIdUTF8 && deviceUniqueIdUTF8Length > 0) {
    memset(deviceUniqueIdUTF8, 0, deviceUniqueIdUTF8Length);
  }
  if (productUniqueIdUTF8 && productUniqueIdUTF8Length > 0) {
    memset(productUniqueIdUTF8, 0, productUniqueIdUTF8Length);
  }

  if (desktop_device_info_->getApplicationInfo(deviceNumber,
                                               desktopApplication) == 0) {
    size_t len;

    const char* deviceName = desktopApplication.getProcessAppName();
    len = deviceName ? strlen(deviceName) : 0;
    if (len && len <= deviceNameUTF8Length) {
      memcpy(deviceNameUTF8, deviceName, len);
    }

    const char* deviceUniqueId = desktopApplication.getUniqueIdName();
    len = deviceUniqueId ? strlen(deviceUniqueId) : 0;
    if (len && deviceUniqueIdUTF8 && len <= deviceUniqueIdUTF8Length) {
      memcpy(deviceUniqueIdUTF8, deviceUniqueId, len);
    }
    if (pid) {
      *pid = desktopApplication.getProcessId();
    }
  }
  return 0;
}

int32_t AppDeviceInfoImpl::DisplayCaptureSettingsDialogBox(
    const char* deviceUniqueIdUTF8, const char* dialogTitleUTF8,
    void* parentWindow, uint32_t positionX, uint32_t positionY) {
  return 0;
}

int32_t AppDeviceInfoImpl::NumberOfCapabilities(
    const char* deviceUniqueIdUTF8) {
  return 0;
}

int32_t AppDeviceInfoImpl::GetCapability(const char* deviceUniqueIdUTF8,
                                         const uint32_t deviceCapabilityNumber,
                                         VideoCaptureCapability& capability) {
  return 0;
}

int32_t AppDeviceInfoImpl::GetBestMatchedCapability(
    const char* deviceUniqueIdUTF8, const VideoCaptureCapability& requested,
    VideoCaptureCapability& resulting) {
  return 0;
}

int32_t AppDeviceInfoImpl::GetOrientation(const char* deviceUniqueIdUTF8,
                                          VideoRotation& orientation) {
  return 0;
}

VideoCaptureModule* DesktopCaptureImpl::Create(const int32_t id,
                                               const char* uniqueId,
                                               const CaptureDeviceType type) {
  return new rtc::RefCountedObject<DesktopCaptureImpl>(id, uniqueId, type);
}

int32_t WindowDeviceInfoImpl::Init() {
  desktop_device_info_ =
      std::unique_ptr<DesktopDeviceInfo>(DesktopDeviceInfoImpl::Create());
  return 0;
}

int32_t WindowDeviceInfoImpl::Refresh() {
  desktop_device_info_->Refresh();
  return 0;
}

uint32_t WindowDeviceInfoImpl::NumberOfDevices() {
  return desktop_device_info_->getWindowCount();
}

int32_t WindowDeviceInfoImpl::GetDeviceName(
    uint32_t deviceNumber, char* deviceNameUTF8, uint32_t deviceNameUTF8Length,
    char* deviceUniqueIdUTF8, uint32_t deviceUniqueIdUTF8Length,
    char* productUniqueIdUTF8, uint32_t productUniqueIdUTF8Length, pid_t* pid) {
  DesktopDisplayDevice desktopDisplayDevice;

  // always initialize output
  if (deviceNameUTF8 && deviceNameUTF8Length > 0) {
    memset(deviceNameUTF8, 0, deviceNameUTF8Length);
  }
  if (deviceUniqueIdUTF8 && deviceUniqueIdUTF8Length > 0) {
    memset(deviceUniqueIdUTF8, 0, deviceUniqueIdUTF8Length);
  }
  if (productUniqueIdUTF8 && productUniqueIdUTF8Length > 0) {
    memset(productUniqueIdUTF8, 0, productUniqueIdUTF8Length);
  }

  if (desktop_device_info_->getWindowInfo(deviceNumber, desktopDisplayDevice) ==
      0) {
    size_t len;

    const char* deviceName = desktopDisplayDevice.getDeviceName();
    len = deviceName ? strlen(deviceName) : 0;
    if (len && deviceNameUTF8 && len <= deviceNameUTF8Length) {
      memcpy(deviceNameUTF8, deviceName, len);
    }

    const char* deviceUniqueId = desktopDisplayDevice.getUniqueIdName();
    len = deviceUniqueId ? strlen(deviceUniqueId) : 0;
    if (len && deviceUniqueIdUTF8 && len <= deviceUniqueIdUTF8Length) {
      memcpy(deviceUniqueIdUTF8, deviceUniqueId, len);
    }
    if (pid) {
      *pid = desktopDisplayDevice.getPid();
    }
  }

  return 0;
}

int32_t WindowDeviceInfoImpl::DisplayCaptureSettingsDialogBox(
    const char* deviceUniqueIdUTF8, const char* dialogTitleUTF8,
    void* parentWindow, uint32_t positionX, uint32_t positionY) {
  // no device properties to change
  return 0;
}

int32_t WindowDeviceInfoImpl::NumberOfCapabilities(
    const char* deviceUniqueIdUTF8) {
  return 0;
}

int32_t WindowDeviceInfoImpl::GetCapability(
    const char* deviceUniqueIdUTF8, const uint32_t deviceCapabilityNumber,
    VideoCaptureCapability& capability) {
  return 0;
}

int32_t WindowDeviceInfoImpl::GetBestMatchedCapability(
    const char* deviceUniqueIdUTF8, const VideoCaptureCapability& requested,
    VideoCaptureCapability& resulting) {
  return 0;
}

int32_t WindowDeviceInfoImpl::GetOrientation(const char* deviceUniqueIdUTF8,
                                             VideoRotation& orientation) {
  return 0;
}

VideoCaptureModule::DeviceInfo* DesktopCaptureImpl::CreateDeviceInfo(
    const int32_t id, const CaptureDeviceType type) {
  if (type == CaptureDeviceType::Screen) {
    ScreenDeviceInfoImpl* pScreenDeviceInfoImpl = new ScreenDeviceInfoImpl(id);
    if (!pScreenDeviceInfoImpl || pScreenDeviceInfoImpl->Init()) {
      delete pScreenDeviceInfoImpl;
      pScreenDeviceInfoImpl = NULL;
    }
    return pScreenDeviceInfoImpl;
  } else if (type == CaptureDeviceType::Window) {
    WindowDeviceInfoImpl* pWindowDeviceInfoImpl = new WindowDeviceInfoImpl(id);
    if (!pWindowDeviceInfoImpl || pWindowDeviceInfoImpl->Init()) {
      delete pWindowDeviceInfoImpl;
      pWindowDeviceInfoImpl = NULL;
    }
    return pWindowDeviceInfoImpl;
  }
  return NULL;
}

const char* DesktopCaptureImpl::CurrentDeviceName() const {
  return _deviceUniqueId.c_str();
}

int32_t DesktopCaptureImpl::Init() {
  // Already initialized
  if (desktop_capturer_cursor_composer_) {
    return 0;
  }

  DesktopCaptureOptions options = DesktopCaptureOptions::CreateDefault();
  // Leave desktop effects enabled during WebRTC captures.
  options.set_disable_effects(false);

  if (_deviceType == CaptureDeviceType::Screen) {
    std::unique_ptr<DesktopCapturer> pScreenCapturer =
        DesktopCapturer::CreateScreenCapturer(options);
    if (!pScreenCapturer.get()) {
      return -1;
    }

    DesktopCapturer::SourceId sourceId = atoi(_deviceUniqueId.c_str());
    pScreenCapturer->SelectSource(sourceId);

    MouseCursorMonitor* pMouseCursorMonitor =
        MouseCursorMonitor::CreateForScreen(options, sourceId);
    desktop_capturer_cursor_composer_ =
        std::unique_ptr<DesktopAndCursorComposer>(new DesktopAndCursorComposer(
            pScreenCapturer.release(), pMouseCursorMonitor));
  } else if (_deviceType == CaptureDeviceType::Window) {
    std::unique_ptr<DesktopCapturer> pWindowCapturer =
        DesktopCapturer::CreateWindowCapturer(options);
    if (!pWindowCapturer.get()) {
      return -1;
    }

    DesktopCapturer::SourceId sourceId = atoi(_deviceUniqueId.c_str());
    pWindowCapturer->SelectSource(sourceId);

    MouseCursorMonitor* pMouseCursorMonitor =
        MouseCursorMonitor::CreateForWindow(
            webrtc::DesktopCaptureOptions::CreateDefault(), sourceId);
    desktop_capturer_cursor_composer_ =
        std::unique_ptr<DesktopAndCursorComposer>(new DesktopAndCursorComposer(
            pWindowCapturer.release(), pMouseCursorMonitor));
  }

  return 0;
}

DesktopCaptureImpl::DesktopCaptureImpl(const int32_t id, const char* uniqueId,
                                       const CaptureDeviceType type)
    : _id(id),
      _deviceUniqueId(uniqueId),
      _deviceType(type),
      _requestedCapability(),
      _rotateFrame(kVideoRotation_0),
      last_capture_time_(rtc::TimeNanos() / rtc::kNumNanosecsPerMillisec),
      // XXX Note that this won't capture drift!
      delta_ntp_internal_ms_(
          Clock::GetRealTimeClock()->CurrentNtpInMilliseconds() -
          last_capture_time_),
      time_event_(EventWrapper::Create()),
#if defined(_WIN32)
      capturer_thread_(
          new rtc::PlatformUIThread(Run, this, "ScreenCaptureThread")),
#else
#  if defined(WEBRTC_LINUX)
      capturer_thread_(nullptr),
#  else
      capturer_thread_(
          new rtc::PlatformThread(Run, this, "ScreenCaptureThread")),
#  endif
#endif
      started_(false) {
  //-> TODO @@NG why is this crashing (seen on Linux)
  //-> capturer_thread_->SetPriority(rtc::kHighPriority);
  _requestedCapability.width = kDefaultWidth;
  _requestedCapability.height = kDefaultHeight;
  _requestedCapability.maxFPS = 30;
  _requestedCapability.videoType = kI420;
  memset(_incomingFrameTimesNanos, 0, sizeof(_incomingFrameTimesNanos));
}

DesktopCaptureImpl::~DesktopCaptureImpl() {
  time_event_->Set();
  if (capturer_thread_) {
    capturer_thread_->Stop();
  }
}

void DesktopCaptureImpl::RegisterCaptureDataCallback(
    rtc::VideoSinkInterface<VideoFrame>* dataCallback) {
  rtc::CritScope lock(&_apiCs);
  rtc::CritScope lock2(&_callBackCs);
  _dataCallBacks.insert(dataCallback);
}

void DesktopCaptureImpl::DeRegisterCaptureDataCallback(
    rtc::VideoSinkInterface<VideoFrame>* dataCallback) {
  rtc::CritScope lock(&_apiCs);
  rtc::CritScope lock2(&_callBackCs);
  auto it = _dataCallBacks.find(dataCallback);
  if (it != _dataCallBacks.end()) {
    _dataCallBacks.erase(it);
  }
}

int32_t DesktopCaptureImpl::StopCaptureIfAllClientsClose() {
  if (_dataCallBacks.empty()) {
    return StopCapture();
  } else {
    return 0;
  }
}

int32_t DesktopCaptureImpl::DeliverCapturedFrame(
    webrtc::VideoFrame& captureFrame, int64_t capture_time) {
  UpdateFrameCount();  // frame count used for local frame rate callBack.

  // Set the capture time
  if (capture_time != 0) {
    captureFrame.set_timestamp_us(1000 *
                                  (capture_time - delta_ntp_internal_ms_));
  } else {
    captureFrame.set_timestamp_us(rtc::TimeMicros());
  }

  if (captureFrame.render_time_ms() == last_capture_time_) {
    // We don't allow the same capture time for two frames, drop this one.
    return -1;
  }
  last_capture_time_ = captureFrame.render_time_ms();

  for (auto dataCallBack : _dataCallBacks) {
    dataCallBack->OnFrame(captureFrame);
  }

  return 0;
}

// Copied from VideoCaptureImpl::IncomingFrame. See Bug 1038324
int32_t DesktopCaptureImpl::IncomingFrame(
    uint8_t* videoFrame, size_t videoFrameLength,
    const VideoCaptureCapability& frameInfo, int64_t captureTime /*=0*/) {
  int64_t startProcessTime = rtc::TimeNanos();
  rtc::CritScope cs(&_callBackCs);

  const int32_t width = frameInfo.width;
  const int32_t height = frameInfo.height;

  // Not encoded, convert to I420.
  if (frameInfo.videoType != VideoType::kMJPEG &&
      CalcBufferSize(frameInfo.videoType, width, abs(height)) !=
          videoFrameLength) {
    RTC_LOG(LS_ERROR) << "Wrong incoming frame length.";
    return -1;
  }

  int stride_y = width;
  int stride_uv = (width + 1) / 2;

  // Setting absolute height (in case it was negative).
  // In Windows, the image starts bottom left, instead of top left.
  // Setting a negative source height, inverts the image (within LibYuv).

  // TODO(nisse): Use a pool?
  rtc::scoped_refptr<I420Buffer> buffer =
      I420Buffer::Create(width, abs(height), stride_y, stride_uv, stride_uv);

  const int conversionResult = libyuv::ConvertToI420(
      videoFrame, videoFrameLength, buffer.get()->MutableDataY(),
      buffer.get()->StrideY(), buffer.get()->MutableDataU(),
      buffer.get()->StrideU(), buffer.get()->MutableDataV(),
      buffer.get()->StrideV(), 0, 0,  // No Cropping
      width, height, width, height, libyuv::kRotate0,
      ConvertVideoType(frameInfo.videoType));
  if (conversionResult != 0) {
    RTC_LOG(LS_ERROR) << "Failed to convert capture frame from type "
                      << static_cast<int>(frameInfo.videoType) << "to I420.";
    return -1;
  }

  VideoFrame captureFrame(buffer, 0, rtc::TimeMillis(), kVideoRotation_0);
  captureFrame.set_ntp_time_ms(captureTime);

  DeliverCapturedFrame(captureFrame, captureTime);

  const int64_t processTime =
      (rtc::TimeNanos() - startProcessTime) / rtc::kNumNanosecsPerMillisec;

  if (processTime > 10) {
    RTC_LOG(LS_WARNING) << "Too long processing time of incoming frame: "
                        << processTime << " ms";
  }

  return 0;
}

int32_t DesktopCaptureImpl::SetCaptureRotation(VideoRotation rotation) {
  rtc::CritScope lock(&_apiCs);
  rtc::CritScope lock2(&_callBackCs);
  _rotateFrame = rotation;
  return 0;
}

bool DesktopCaptureImpl::SetApplyRotation(bool enable) { return true; }

void DesktopCaptureImpl::UpdateFrameCount() {
  if (_incomingFrameTimesNanos[0] == 0) {
    // first no shift
  } else {
    // shift
    for (int i = (kFrameRateCountHistorySize - 2); i >= 0; i--) {
      _incomingFrameTimesNanos[i + 1] = _incomingFrameTimesNanos[i];
    }
  }
  _incomingFrameTimesNanos[0] = rtc::TimeNanos();
}

uint32_t DesktopCaptureImpl::CalculateFrameRate(int64_t now_ns) {
  int32_t num = 0;
  int32_t nrOfFrames = 0;
  for (num = 1; num < (kFrameRateCountHistorySize - 1); num++) {
    if (_incomingFrameTimesNanos[num] <= 0 ||
        (now_ns - _incomingFrameTimesNanos[num]) /
                rtc::kNumNanosecsPerMillisec >
            kFrameRateHistoryWindowMs)  // don't use data older than 2sec
    {
      break;
    } else {
      nrOfFrames++;
    }
  }
  if (num > 1) {
    int64_t diff = (now_ns - _incomingFrameTimesNanos[num - 1]) /
                   rtc::kNumNanosecsPerMillisec;
    if (diff > 0) {
      return uint32_t((nrOfFrames * 1000.0f / diff) + 0.5f);
    }
  }

  return nrOfFrames;
}

int32_t DesktopCaptureImpl::StartCapture(
    const VideoCaptureCapability& capability) {
  _requestedCapability = capability;
#if defined(_WIN32)
  uint32_t maxFPSNeeded = _requestedCapability.maxFPS > 0
                              ? 1000 / _requestedCapability.maxFPS
                              : 1000;
  capturer_thread_->RequestCallbackTimer(maxFPSNeeded);
#endif

  if (started_) {
    return 0;
  }
#if defined(WEBRTC_LINUX)
  // Lazily init capturer_thread_
  if (!capturer_thread_) {
    capturer_thread_ = std::unique_ptr<rtc::PlatformThread>(
        new rtc::PlatformThread(Run, this, "ScreenCaptureThread"));
  }
#endif

  uint32_t err = Init();
  if (err) {
    return err;
  }

  desktop_capturer_cursor_composer_->Start(this);
  capturer_thread_->Start();
  started_ = true;

  return 0;
}

bool DesktopCaptureImpl::FocusOnSelectedSource() {
  uint32_t err = Init();
  if (err) {
    return false;
  }

  return desktop_capturer_cursor_composer_->FocusOnSelectedSource();
}

int32_t DesktopCaptureImpl::StopCapture() {
  if (started_) {
    capturer_thread_
        ->Stop();  // thread is guaranteed stopped before this returns
    desktop_capturer_cursor_composer_.reset();
    started_ = false;
    return 0;
  }
  return -1;
}

bool DesktopCaptureImpl::CaptureStarted() { return started_; }

int32_t DesktopCaptureImpl::CaptureSettings(VideoCaptureCapability& settings) {
  return -1;
}

void DesktopCaptureImpl::OnCaptureResult(DesktopCapturer::Result result,
                                         std::unique_ptr<DesktopFrame> frame) {
  if (frame.get() == nullptr) return;
  uint8_t* videoFrame = frame->data();
  VideoCaptureCapability frameInfo;
  frameInfo.width = frame->size().width();
  frameInfo.height = frame->size().height();
  frameInfo.videoType = VideoType::kARGB;

  size_t videoFrameLength =
      frameInfo.width * frameInfo.height * DesktopFrame::kBytesPerPixel;
  IncomingFrame(videoFrame, videoFrameLength, frameInfo);
}

void DesktopCaptureImpl::process() {
  DesktopRect desktop_rect;
  DesktopRegion desktop_region;

#if !defined(_WIN32)
  int64_t startProcessTime = rtc::TimeNanos();
#endif

  desktop_capturer_cursor_composer_->CaptureFrame();

#if !defined(_WIN32)
  const uint32_t processTime =
      ((uint32_t)(rtc::TimeNanos() - startProcessTime)) /
      rtc::kNumNanosecsPerMillisec;
  // Use at most x% CPU or limit framerate
  const uint32_t maxFPSNeeded = _requestedCapability.maxFPS > 0
                                    ? 1000 / _requestedCapability.maxFPS
                                    : 1000;
  const float sleepTimeFactor = (100.0f / kMaxDesktopCaptureCpuUsage) - 1.0f;
  const uint32_t sleepTime = sleepTimeFactor * processTime;
  time_event_->Wait(std::max<uint32_t>(maxFPSNeeded, sleepTime));
#endif
}

}  // namespace webrtc
