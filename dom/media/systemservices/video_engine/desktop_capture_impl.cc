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
#include <memory>
#include <string>

#include "api/video/i420_buffer.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "libyuv.h"  // NOLINT
#include "modules/include/module_common_types.h"
#include "modules/video_capture/video_capture_config.h"
#include "system_wrappers/include/clock.h"
#include "rtc_base/logging.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/trace_event.h"
#include "video_engine/desktop_capture_impl.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/video_capture/video_capture.h"
#include "mozilla/StaticPrefs_media.h"

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
    uint32_t deviceNumber, char* deviceNameUTF8, uint32_t deviceNameUTF8Size,
    char* deviceUniqueIdUTF8, uint32_t deviceUniqueIdUTF8Size,
    char* productUniqueIdUTF8, uint32_t productUniqueIdUTF8Size, pid_t* pid) {
  DesktopDisplayDevice desktopDisplayDevice;

  // always initialize output
  if (deviceNameUTF8 && deviceNameUTF8Size > 0) {
    memset(deviceNameUTF8, 0, deviceNameUTF8Size);
  }

  if (deviceUniqueIdUTF8 && deviceUniqueIdUTF8Size > 0) {
    memset(deviceUniqueIdUTF8, 0, deviceUniqueIdUTF8Size);
  }
  if (productUniqueIdUTF8 && productUniqueIdUTF8Size > 0) {
    memset(productUniqueIdUTF8, 0, productUniqueIdUTF8Size);
  }

  if (desktop_device_info_->getDesktopDisplayDeviceInfo(
          deviceNumber, desktopDisplayDevice) == 0) {
    size_t len;

    const char* deviceName = desktopDisplayDevice.getDeviceName();
    len = deviceName ? strlen(deviceName) : 0;
    if (len && deviceNameUTF8 && len < deviceNameUTF8Size) {
      memcpy(deviceNameUTF8, deviceName, len);
    }

    const char* deviceUniqueId = desktopDisplayDevice.getUniqueIdName();
    len = deviceUniqueId ? strlen(deviceUniqueId) : 0;
    if (len && deviceUniqueIdUTF8 && len < deviceUniqueIdUTF8Size) {
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
    uint32_t deviceNumber, char* deviceNameUTF8, uint32_t deviceNameUTF8Size,
    char* deviceUniqueIdUTF8, uint32_t deviceUniqueIdUTF8Size,
    char* productUniqueIdUTF8, uint32_t productUniqueIdUTF8Size, pid_t* pid) {
  DesktopDisplayDevice desktopDisplayDevice;

  // always initialize output
  if (deviceNameUTF8 && deviceNameUTF8Size > 0) {
    memset(deviceNameUTF8, 0, deviceNameUTF8Size);
  }
  if (deviceUniqueIdUTF8 && deviceUniqueIdUTF8Size > 0) {
    memset(deviceUniqueIdUTF8, 0, deviceUniqueIdUTF8Size);
  }
  if (productUniqueIdUTF8 && productUniqueIdUTF8Size > 0) {
    memset(productUniqueIdUTF8, 0, productUniqueIdUTF8Size);
  }

  if (desktop_device_info_->getWindowInfo(deviceNumber, desktopDisplayDevice) ==
      0) {
    size_t len;

    const char* deviceName = desktopDisplayDevice.getDeviceName();
    len = deviceName ? strlen(deviceName) : 0;
    if (len && deviceNameUTF8 && len < deviceNameUTF8Size) {
      memcpy(deviceNameUTF8, deviceName, len);
    }

    const char* deviceUniqueId = desktopDisplayDevice.getUniqueIdName();
    len = deviceUniqueId ? strlen(deviceUniqueId) : 0;
    if (len && deviceUniqueIdUTF8 && len < deviceUniqueIdUTF8Size) {
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

int32_t BrowserDeviceInfoImpl::Init() {
  desktop_device_info_ =
      std::unique_ptr<DesktopDeviceInfo>(DesktopDeviceInfoImpl::Create());
  return 0;
}

int32_t BrowserDeviceInfoImpl::Refresh() {
  desktop_device_info_->Refresh();
  return 0;
}

uint32_t BrowserDeviceInfoImpl::NumberOfDevices() {
  return desktop_device_info_->getTabCount();
}

int32_t BrowserDeviceInfoImpl::GetDeviceName(
    uint32_t deviceNumber, char* deviceNameUTF8, uint32_t deviceNameUTF8Size,
    char* deviceUniqueIdUTF8, uint32_t deviceUniqueIdUTF8Size,
    char* productUniqueIdUTF8, uint32_t productUniqueIdUTF8Size, pid_t* pid) {
  DesktopTab desktopTab;

  // always initialize output
  if (deviceNameUTF8 && deviceNameUTF8Size > 0) {
    memset(deviceNameUTF8, 0, deviceNameUTF8Size);
  }
  if (deviceUniqueIdUTF8 && deviceUniqueIdUTF8Size > 0) {
    memset(deviceUniqueIdUTF8, 0, deviceUniqueIdUTF8Size);
  }
  if (productUniqueIdUTF8 && productUniqueIdUTF8Size > 0) {
    memset(productUniqueIdUTF8, 0, productUniqueIdUTF8Size);
  }

  if (desktop_device_info_->getTabInfo(deviceNumber, desktopTab) == 0) {
    size_t len;

    const char* deviceName = desktopTab.getTabName();
    len = deviceName ? strlen(deviceName) : 0;
    if (len && deviceNameUTF8 && len < deviceNameUTF8Size) {
      memcpy(deviceNameUTF8, deviceName, len);
    }

    const char* deviceUniqueId = desktopTab.getUniqueIdName();
    len = deviceUniqueId ? strlen(deviceUniqueId) : 0;
    if (len && deviceUniqueIdUTF8 && len < deviceUniqueIdUTF8Size) {
      memcpy(deviceUniqueIdUTF8, deviceUniqueId, len);
    }
  }

  return 0;
}

int32_t BrowserDeviceInfoImpl::DisplayCaptureSettingsDialogBox(
    const char* deviceUniqueIdUTF8, const char* dialogTitleUTF8,
    void* parentWindow, uint32_t positionX, uint32_t positionY) {
  // no device properties to change
  return 0;
}

int32_t BrowserDeviceInfoImpl::NumberOfCapabilities(
    const char* deviceUniqueIdUTF8) {
  return 0;
}

int32_t BrowserDeviceInfoImpl::GetCapability(
    const char* deviceUniqueIdUTF8, const uint32_t deviceCapabilityNumber,
    VideoCaptureCapability& capability) {
  return 0;
}

int32_t BrowserDeviceInfoImpl::GetBestMatchedCapability(
    const char* deviceUniqueIdUTF8, const VideoCaptureCapability& requested,
    VideoCaptureCapability& resulting) {
  return 0;
}

int32_t BrowserDeviceInfoImpl::GetOrientation(const char* deviceUniqueIdUTF8,
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
  } else if (type == CaptureDeviceType::Browser) {
    BrowserDeviceInfoImpl* pBrowserDeviceInfoImpl =
        new BrowserDeviceInfoImpl(id);
    if (!pBrowserDeviceInfoImpl || pBrowserDeviceInfoImpl->Init()) {
      delete pBrowserDeviceInfoImpl;
      pBrowserDeviceInfoImpl = NULL;
    }
    return pBrowserDeviceInfoImpl;
  }
  return NULL;
}

const char* DesktopCaptureImpl::CurrentDeviceName() const {
  return _deviceUniqueId.c_str();
}

int32_t DesktopCaptureImpl::LazyInitDesktopCapturer() {
  // Already initialized
  if (desktop_capturer_cursor_composer_) {
    return 0;
  }

  DesktopCaptureOptions options = DesktopCaptureOptions::CreateDefault();
  // Leave desktop effects enabled during WebRTC captures.
  options.set_disable_effects(false);

#if defined(WEBRTC_MAC)
  if (mozilla::StaticPrefs::media_webrtc_capture_allow_iosurface()) {
    options.set_allow_iosurface(true);
  }
#endif

  if (_deviceType == CaptureDeviceType::Screen) {
    std::unique_ptr<DesktopCapturer> pScreenCapturer =
        DesktopCapturer::CreateScreenCapturer(options);
    if (!pScreenCapturer.get()) {
      return -1;
    }

    DesktopCapturer::SourceId sourceId = atoi(_deviceUniqueId.c_str());
    pScreenCapturer->SelectSource(sourceId);

    desktop_capturer_cursor_composer_ =
        std::unique_ptr<DesktopAndCursorComposer>(
            new DesktopAndCursorComposer(std::move(pScreenCapturer), options));
  } else if (_deviceType == CaptureDeviceType::Window) {
    std::unique_ptr<DesktopCapturer> pWindowCapturer =
        DesktopCapturer::CreateWindowCapturer(options);
    if (!pWindowCapturer.get()) {
      return -1;
    }

    DesktopCapturer::SourceId sourceId = atoi(_deviceUniqueId.c_str());
    pWindowCapturer->SelectSource(sourceId);

    desktop_capturer_cursor_composer_ =
        std::unique_ptr<DesktopAndCursorComposer>(
            new DesktopAndCursorComposer(std::move(pWindowCapturer), options));
  } else if (_deviceType == CaptureDeviceType::Browser) {
    // XXX We don't capture cursors, so avoid the extra indirection layer. We
    // could also pass null for the pMouseCursorMonitor.
    desktop_capturer_cursor_composer_ =
        DesktopCapturer::CreateTabCapturer(options);
    if (!desktop_capturer_cursor_composer_) {
      return -1;
    }

    DesktopCapturer::SourceId sourceId = atoi(_deviceUniqueId.c_str());
    desktop_capturer_cursor_composer_->SelectSource(sourceId);
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
      last_capture_time_ms_(rtc::TimeMillis()),
      time_event_(EventWrapper::Create()),
      capturer_thread_(nullptr),
      started_(false) {
  _requestedCapability.width = kDefaultWidth;
  _requestedCapability.height = kDefaultHeight;
  _requestedCapability.maxFPS = 30;
  _requestedCapability.videoType = VideoType::kI420;
  _maxFPSNeeded = 1000 / _requestedCapability.maxFPS;
  memset(_incomingFrameTimesNanos, 0, sizeof(_incomingFrameTimesNanos));
}

DesktopCaptureImpl::~DesktopCaptureImpl() {
  time_event_->Set();
  if (capturer_thread_) {
#if defined(_WIN32)
    capturer_thread_->Stop();
#else
    capturer_thread_->Finalize();
#endif
  }
}

void DesktopCaptureImpl::RegisterCaptureDataCallback(
    rtc::VideoSinkInterface<VideoFrame>* dataCallback) {
  rtc::CritScope lock(&_apiCs);
  _dataCallBacks.insert(dataCallback);
}

void DesktopCaptureImpl::DeRegisterCaptureDataCallback(
    rtc::VideoSinkInterface<VideoFrame>* dataCallback) {
  rtc::CritScope lock(&_apiCs);
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
    webrtc::VideoFrame& captureFrame) {
  UpdateFrameCount();  // frame count used for local frame rate callBack.

  // Set the capture time
  captureFrame.set_timestamp_us(rtc::TimeMicros());

  if (captureFrame.render_time_ms() == last_capture_time_ms_) {
    // We don't allow the same capture time for two frames, drop this one.
    return -1;
  }
  last_capture_time_ms_ = captureFrame.render_time_ms();

  for (auto dataCallBack : _dataCallBacks) {
    dataCallBack->OnFrame(captureFrame);
  }

  return 0;
}

// Originally copied from VideoCaptureImpl::IncomingFrame, but has diverged
// somewhat. See Bug 1038324 and bug 1738946.
int32_t DesktopCaptureImpl::IncomingFrame(
    uint8_t* videoFrame, size_t videoFrameLength, size_t widthWithPadding,
    const VideoCaptureCapability& frameInfo) {
  int64_t startProcessTime = rtc::TimeNanos();
  rtc::CritScope cs(&_apiCs);

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
      static_cast<int>(widthWithPadding), height, width, height,
      libyuv::kRotate0, ConvertVideoType(frameInfo.videoType));
  if (conversionResult != 0) {
    RTC_LOG(LS_ERROR) << "Failed to convert capture frame from type "
                      << static_cast<int>(frameInfo.videoType) << "to I420.";
    return -1;
  }

  VideoFrame captureFrame(buffer, 0, rtc::TimeMillis(), kVideoRotation_0);

  DeliverCapturedFrame(captureFrame);

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

void DesktopCaptureImpl::LazyInitCaptureThread() {
  MOZ_ASSERT(desktop_capturer_cursor_composer_,
             "DesktopCapturer must be initialized before the capture thread");
  if (capturer_thread_) {
    return;
  }
#if defined(_WIN32)
  capturer_thread_ = std::make_unique<rtc::PlatformUIThread>(
      std::function([self = (void*)this]() { Run(self); }),
      "ScreenCaptureThread", rtc::ThreadAttributes{});
  capturer_thread_->RequestCallbackTimer(_maxFPSNeeded);
#else
  auto self = rtc::scoped_refptr<DesktopCaptureImpl>(this);
  capturer_thread_ =
      std::make_unique<rtc::PlatformThread>(rtc::PlatformThread::SpawnJoinable(
          [self] { self->process(); }, "ScreenCaptureThread"));
#endif
  started_ = true;
}

int32_t DesktopCaptureImpl::StartCapture(
    const VideoCaptureCapability& capability) {
  rtc::CritScope lock(&_apiCs);
  // See Bug 1780884 for followup on understanding why multiple calls happen.
  // MOZ_ASSERT(!started_, "Capture must be stopped before Start() can be
  // called");
  if (started_) {
    return 0;
  }

  if (uint32_t err = LazyInitDesktopCapturer(); err) {
    return err;
  }
  started_ = true;
  _requestedCapability = capability;
  _maxFPSNeeded = _requestedCapability.maxFPS > 0
                      ? 1000 / _requestedCapability.maxFPS
                      : 1000;
  LazyInitCaptureThread();

  return 0;
}

bool DesktopCaptureImpl::FocusOnSelectedSource() {
  if (uint32_t err = LazyInitDesktopCapturer(); err) {
    return false;
  }

  return desktop_capturer_cursor_composer_->FocusOnSelectedSource();
}

int32_t DesktopCaptureImpl::StopCapture() {
  if (started_) {
    started_ = false;
    MOZ_ASSERT(capturer_thread_, "Capturer thread should be initialized.");

#if defined(_WIN32)
    capturer_thread_
        ->Stop();  // thread is guaranteed stopped before this returns
#else
    capturer_thread_
        ->Finalize();  // thread is guaranteed stopped before this returns
#endif
    desktop_capturer_cursor_composer_.reset();
    cursor_composer_started_ = false;
    capturer_thread_.reset();
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
  IncomingFrame(videoFrame, videoFrameLength,
                frame->stride() / DesktopFrame::kBytesPerPixel, frameInfo);
}

void DesktopCaptureImpl::process() {
  // We need to call Start on the same thread we call CaptureFrame on.
  if (!cursor_composer_started_) {
    desktop_capturer_cursor_composer_->Start(this);
    cursor_composer_started_ = true;
  }
#if defined(WEBRTC_WIN)
  ProcessIter();
#else
  do {
    ProcessIter();
  } while (started_);
#endif
}

void DesktopCaptureImpl::ProcessIter() {
  // We should deliver at least one frame before stopping
#if !defined(_WIN32)
  int64_t startProcessTime = rtc::TimeNanos();
#endif

#if defined(WEBRTC_MAC)
  // Give cycles to the RunLoop so frame callbacks can happen
  CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, true);
#endif

  desktop_capturer_cursor_composer_->CaptureFrame();

#if !defined(_WIN32)
  const uint32_t processTime =
      ((uint32_t)(rtc::TimeNanos() - startProcessTime)) /
      rtc::kNumNanosecsPerMillisec;
  // Use at most x% CPU or limit framerate
  const float sleepTimeFactor = (100.0f / kMaxDesktopCaptureCpuUsage) - 1.0f;
  const uint32_t sleepTime = sleepTimeFactor * processTime;
  time_event_->Wait(std::max<uint32_t>(_maxFPSNeeded, sleepTime));
#endif

#if defined(WEBRTC_WIN)
// Let the timer events in PlatformUIThread drive process,
// don't sleep.
#elif defined(WEBRTC_MAC)
  sched_yield();
#else
  static const struct timespec ts_null = {0};
  nanosleep(&ts_null, nullptr);
#endif
}

}  // namespace webrtc
