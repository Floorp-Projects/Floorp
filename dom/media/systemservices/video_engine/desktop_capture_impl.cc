/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/desktop_capture_impl.h"

#include <stdlib.h>
#include <memory>
#include <string>

#include "CamerasTypes.h"
#include "PerformanceRecorder.h"

#include "api/video/i420_buffer.h"
#include "base/scoped_nsautorelease_pool.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "libyuv.h"  // NOLINT
#include "modules/include/module_common_types.h"
#include "modules/video_capture/video_capture_config.h"
#include "modules/video_capture/video_capture_impl.h"
#include "system_wrappers/include/clock.h"
#include "rtc_base/logging.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/trace_event.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/video_capture/video_capture.h"
#include "mozilla/Maybe.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/SyncRunnable.h"

#include "PerformanceRecorder.h"

#if defined(_WIN32)
#  include "platform_uithread.h"
#else
#  include "rtc_base/platform_thread.h"
#endif

namespace webrtc {

int32_t ScreenDeviceInfoImpl::Init() {
  mDesktopDeviceInfo =
      std::unique_ptr<DesktopDeviceInfo>(DesktopDeviceInfo::Create());
  return 0;
}

int32_t ScreenDeviceInfoImpl::Refresh() {
  mDesktopDeviceInfo->Refresh();
  return 0;
}

uint32_t ScreenDeviceInfoImpl::NumberOfDevices() {
  return mDesktopDeviceInfo->getDisplayDeviceCount();
}

int32_t ScreenDeviceInfoImpl::GetDeviceName(
    uint32_t aDeviceNumber, char* aDeviceNameUTF8, uint32_t aDeviceNameUTF8Size,
    char* aDeviceUniqueIdUTF8, uint32_t aDeviceUniqueIdUTF8Size,
    char* aProductUniqueIdUTF8, uint32_t aProductUniqueIdUTF8Size,
    pid_t* aPid) {
  DesktopDisplayDevice desktopDisplayDevice;

  // always initialize output
  if (aDeviceNameUTF8 && aDeviceNameUTF8Size > 0) {
    memset(aDeviceNameUTF8, 0, aDeviceNameUTF8Size);
  }

  if (aDeviceUniqueIdUTF8 && aDeviceUniqueIdUTF8Size > 0) {
    memset(aDeviceUniqueIdUTF8, 0, aDeviceUniqueIdUTF8Size);
  }
  if (aProductUniqueIdUTF8 && aProductUniqueIdUTF8Size > 0) {
    memset(aProductUniqueIdUTF8, 0, aProductUniqueIdUTF8Size);
  }

  if (mDesktopDeviceInfo->getDesktopDisplayDeviceInfo(
          aDeviceNumber, desktopDisplayDevice) == 0) {
    size_t len;

    const char* deviceName = desktopDisplayDevice.getDeviceName();
    len = deviceName ? strlen(deviceName) : 0;
    if (len && aDeviceNameUTF8 && len < aDeviceNameUTF8Size) {
      memcpy(aDeviceNameUTF8, deviceName, len);
    }

    const char* deviceUniqueId = desktopDisplayDevice.getUniqueIdName();
    len = deviceUniqueId ? strlen(deviceUniqueId) : 0;
    if (len && aDeviceUniqueIdUTF8 && len < aDeviceUniqueIdUTF8Size) {
      memcpy(aDeviceUniqueIdUTF8, deviceUniqueId, len);
    }
  }

  return 0;
}

int32_t ScreenDeviceInfoImpl::DisplayCaptureSettingsDialogBox(
    const char* aDeviceUniqueIdUTF8, const char* aDialogTitleUTF8,
    void* aParentWindow, uint32_t aPositionX, uint32_t aPositionY) {
  // no device properties to change
  return 0;
}

int32_t ScreenDeviceInfoImpl::NumberOfCapabilities(
    const char* aDeviceUniqueIdUTF8) {
  return 0;
}

int32_t ScreenDeviceInfoImpl::GetCapability(
    const char* aDeviceUniqueIdUTF8, uint32_t aDeviceCapabilityNumber,
    VideoCaptureCapability& aCapability) {
  return 0;
}

int32_t ScreenDeviceInfoImpl::GetBestMatchedCapability(
    const char* aDeviceUniqueIdUTF8, const VideoCaptureCapability& aRequested,
    VideoCaptureCapability& aResulting) {
  return 0;
}

int32_t ScreenDeviceInfoImpl::GetOrientation(const char* aDeviceUniqueIdUTF8,
                                             VideoRotation& aOrientation) {
  return 0;
}

VideoCaptureModule* DesktopCaptureImpl::Create(const int32_t aModuleId,
                                               const char* aUniqueId,
                                               const CaptureDeviceType aType) {
  return new rtc::RefCountedObject<DesktopCaptureImpl>(aModuleId, aUniqueId,
                                                       aType);
}

int32_t WindowDeviceInfoImpl::Init() {
  mDesktopDeviceInfo =
      std::unique_ptr<DesktopDeviceInfo>(DesktopDeviceInfo::Create());
  return 0;
}

int32_t WindowDeviceInfoImpl::Refresh() {
  mDesktopDeviceInfo->Refresh();
  return 0;
}

uint32_t WindowDeviceInfoImpl::NumberOfDevices() {
  return mDesktopDeviceInfo->getWindowCount();
}

int32_t WindowDeviceInfoImpl::GetDeviceName(
    uint32_t aDeviceNumber, char* aDeviceNameUTF8, uint32_t aDeviceNameUTF8Size,
    char* aDeviceUniqueIdUTF8, uint32_t aDeviceUniqueIdUTF8Size,
    char* aProductUniqueIdUTF8, uint32_t aProductUniqueIdUTF8Size,
    pid_t* aPid) {
  DesktopDisplayDevice desktopDisplayDevice;

  // always initialize output
  if (aDeviceNameUTF8 && aDeviceNameUTF8Size > 0) {
    memset(aDeviceNameUTF8, 0, aDeviceNameUTF8Size);
  }
  if (aDeviceUniqueIdUTF8 && aDeviceUniqueIdUTF8Size > 0) {
    memset(aDeviceUniqueIdUTF8, 0, aDeviceUniqueIdUTF8Size);
  }
  if (aProductUniqueIdUTF8 && aProductUniqueIdUTF8Size > 0) {
    memset(aProductUniqueIdUTF8, 0, aProductUniqueIdUTF8Size);
  }

  if (mDesktopDeviceInfo->getWindowInfo(aDeviceNumber, desktopDisplayDevice) ==
      0) {
    size_t len;

    const char* deviceName = desktopDisplayDevice.getDeviceName();
    len = deviceName ? strlen(deviceName) : 0;
    if (len && aDeviceNameUTF8 && len < aDeviceNameUTF8Size) {
      memcpy(aDeviceNameUTF8, deviceName, len);
    }

    const char* deviceUniqueId = desktopDisplayDevice.getUniqueIdName();
    len = deviceUniqueId ? strlen(deviceUniqueId) : 0;
    if (len && aDeviceUniqueIdUTF8 && len < aDeviceUniqueIdUTF8Size) {
      memcpy(aDeviceUniqueIdUTF8, deviceUniqueId, len);
    }
    if (aPid) {
      *aPid = desktopDisplayDevice.getPid();
    }
  }

  return 0;
}

int32_t WindowDeviceInfoImpl::DisplayCaptureSettingsDialogBox(
    const char* aDeviceUniqueIdUTF8, const char* aDialogTitleUTF8,
    void* aParentWindow, uint32_t aPositionX, uint32_t aPositionY) {
  // no device properties to change
  return 0;
}

int32_t WindowDeviceInfoImpl::NumberOfCapabilities(
    const char* aDeviceUniqueIdUTF8) {
  return 0;
}

int32_t WindowDeviceInfoImpl::GetCapability(
    const char* aDeviceUniqueIdUTF8, uint32_t aDeviceCapabilityNumber,
    VideoCaptureCapability& aCapability) {
  return 0;
}

int32_t WindowDeviceInfoImpl::GetBestMatchedCapability(
    const char* aDeviceUniqueIdUTF8, const VideoCaptureCapability& aRequested,
    VideoCaptureCapability& aResulting) {
  return 0;
}

int32_t WindowDeviceInfoImpl::GetOrientation(const char* aDeviceUniqueIdUTF8,
                                             VideoRotation& aOrientation) {
  return 0;
}

int32_t BrowserDeviceInfoImpl::Init() {
  mDesktopDeviceInfo =
      std::unique_ptr<DesktopDeviceInfo>(DesktopDeviceInfo::Create());
  return 0;
}

int32_t BrowserDeviceInfoImpl::Refresh() {
  mDesktopDeviceInfo->Refresh();
  return 0;
}

uint32_t BrowserDeviceInfoImpl::NumberOfDevices() {
  return mDesktopDeviceInfo->getTabCount();
}

int32_t BrowserDeviceInfoImpl::GetDeviceName(
    uint32_t aDeviceNumber, char* aDeviceNameUTF8, uint32_t aDeviceNameUTF8Size,
    char* aDeviceUniqueIdUTF8, uint32_t aDeviceUniqueIdUTF8Size,
    char* aProductUniqueIdUTF8, uint32_t aProductUniqueIdUTF8Size,
    pid_t* aPid) {
  DesktopTab desktopTab;

  // always initialize output
  if (aDeviceNameUTF8 && aDeviceNameUTF8Size > 0) {
    memset(aDeviceNameUTF8, 0, aDeviceNameUTF8Size);
  }
  if (aDeviceUniqueIdUTF8 && aDeviceUniqueIdUTF8Size > 0) {
    memset(aDeviceUniqueIdUTF8, 0, aDeviceUniqueIdUTF8Size);
  }
  if (aProductUniqueIdUTF8 && aProductUniqueIdUTF8Size > 0) {
    memset(aProductUniqueIdUTF8, 0, aProductUniqueIdUTF8Size);
  }

  if (mDesktopDeviceInfo->getTabInfo(aDeviceNumber, desktopTab) == 0) {
    size_t len;

    const char* deviceName = desktopTab.getTabName();
    len = deviceName ? strlen(deviceName) : 0;
    if (len && aDeviceNameUTF8 && len < aDeviceNameUTF8Size) {
      memcpy(aDeviceNameUTF8, deviceName, len);
    }

    const char* deviceUniqueId = desktopTab.getUniqueIdName();
    len = deviceUniqueId ? strlen(deviceUniqueId) : 0;
    if (len && aDeviceUniqueIdUTF8 && len < aDeviceUniqueIdUTF8Size) {
      memcpy(aDeviceUniqueIdUTF8, deviceUniqueId, len);
    }
  }

  return 0;
}

int32_t BrowserDeviceInfoImpl::DisplayCaptureSettingsDialogBox(
    const char* aDeviceUniqueIdUTF8, const char* aDialogTitleUTF8,
    void* aParentWindow, uint32_t aPositionX, uint32_t aPositionY) {
  // no device properties to change
  return 0;
}

int32_t BrowserDeviceInfoImpl::NumberOfCapabilities(
    const char* aDeviceUniqueIdUTF8) {
  return 0;
}

int32_t BrowserDeviceInfoImpl::GetCapability(
    const char* aDeviceUniqueIdUTF8, uint32_t aDeviceCapabilityNumber,
    VideoCaptureCapability& aCapability) {
  return 0;
}

int32_t BrowserDeviceInfoImpl::GetBestMatchedCapability(
    const char* aDeviceUniqueIdUTF8, const VideoCaptureCapability& aRequested,
    VideoCaptureCapability& aResulting) {
  return 0;
}

int32_t BrowserDeviceInfoImpl::GetOrientation(const char* aDeviceUniqueIdUTF8,
                                              VideoRotation& aOrientation) {
  return 0;
}

std::shared_ptr<VideoCaptureModule::DeviceInfo>
DesktopCaptureImpl::CreateDeviceInfo(const int32_t aId,
                                     const CaptureDeviceType aType) {
  if (aType == CaptureDeviceType::Screen) {
    auto screenInfo = std::make_shared<ScreenDeviceInfoImpl>(aId);
    if (!screenInfo || screenInfo->Init() != 0) {
      return nullptr;
    }
    return screenInfo;
  }
  if (aType == CaptureDeviceType::Window) {
    auto windowInfo = std::make_shared<WindowDeviceInfoImpl>(aId);
    if (!windowInfo || windowInfo->Init() != 0) {
      return nullptr;
    }
    return windowInfo;
  }
  if (aType == CaptureDeviceType::Browser) {
    auto browserInfo = std::make_shared<BrowserDeviceInfoImpl>(aId);
    if (!browserInfo || browserInfo->Init() != 0) {
      return nullptr;
    }
    return browserInfo;
  }
  return nullptr;
}

const char* DesktopCaptureImpl::CurrentDeviceName() const {
  return mDeviceUniqueId.c_str();
}

static DesktopCaptureOptions CreateDesktopCaptureOptions() {
  DesktopCaptureOptions options;
// Help avoid an X11 deadlock, see bug 1456101.
#ifdef MOZ_X11
  MOZ_ALWAYS_SUCCEEDS(mozilla::SyncRunnable::DispatchToThread(
      mozilla::GetMainThreadSerialEventTarget(),
      NS_NewRunnableFunction(__func__, [&] {
        options = DesktopCaptureOptions::CreateDefault();
      })));
#else
  options = DesktopCaptureOptions::CreateDefault();
#endif

  // Leave desktop effects enabled during WebRTC captures.
  options.set_disable_effects(false);

#if defined(WEBRTC_WIN)
  if (mozilla::StaticPrefs::media_webrtc_capture_allow_directx()) {
    options.set_allow_directx_capturer(true);
    options.set_allow_use_magnification_api(false);
  } else {
    options.set_allow_use_magnification_api(true);
  }
  options.set_allow_cropping_window_capturer(true);
#  if defined(RTC_ENABLE_WIN_WGC)
  if (mozilla::StaticPrefs::media_webrtc_capture_allow_wgc()) {
    options.set_allow_wgc_capturer(true);
  }
#  endif
#endif

#if defined(WEBRTC_MAC)
  if (mozilla::StaticPrefs::media_webrtc_capture_allow_iosurface()) {
    options.set_allow_iosurface(true);
  }
#endif

#if defined(WEBRTC_USE_PIPEWIRE)
  if (mozilla::StaticPrefs::media_webrtc_capture_allow_pipewire()) {
    options.set_allow_pipewire(true);
  }
#endif

  return options;
}

int32_t DesktopCaptureImpl::LazyInitDesktopCapturer() {
  if (mCapturer) {
    // Already initialized

    return 0;
  }

  DesktopCaptureOptions options = CreateDesktopCaptureOptions();

#if defined(WEBRTC_USE_PIPEWIRE)
  if (mozilla::StaticPrefs::media_webrtc_capture_allow_pipewire() &&
      webrtc::DesktopCapturer::IsRunningUnderWayland()) {
    std::unique_ptr<DesktopCapturer> capturer =
        DesktopCapturer::CreateGenericCapturer(options);
    if (!capturer) {
      return -1;
    }

    mCapturer = std::make_unique<DesktopAndCursorComposer>(std::move(capturer),
                                                           options);

    return 0;
  }
#endif

  if (mDeviceType == CaptureDeviceType::Screen) {
    std::unique_ptr<DesktopCapturer> screenCapturer =
        DesktopCapturer::CreateScreenCapturer(options);
    if (!screenCapturer) {
      return -1;
    }

    DesktopCapturer::SourceId sourceId = atoi(mDeviceUniqueId.c_str());
    screenCapturer->SelectSource(sourceId);

    mCapturer = std::make_unique<DesktopAndCursorComposer>(
        std::move(screenCapturer), options);
  } else if (mDeviceType == CaptureDeviceType::Window) {
#if defined(RTC_ENABLE_WIN_WGC)
    options.set_allow_wgc_capturer_fallback(true);
#endif
    std::unique_ptr<DesktopCapturer> windowCapturer =
        DesktopCapturer::CreateWindowCapturer(options);
    if (!windowCapturer) {
      return -1;
    }

    DesktopCapturer::SourceId sourceId = atoi(mDeviceUniqueId.c_str());
    windowCapturer->SelectSource(sourceId);

    mCapturer = std::make_unique<DesktopAndCursorComposer>(
        std::move(windowCapturer), options);
  } else if (mDeviceType == CaptureDeviceType::Browser) {
    // XXX We don't capture cursors, so avoid the extra indirection layer. We
    // could also pass null for the pMouseCursorMonitor.
    mCapturer = DesktopCapturer::CreateTabCapturer(options);
    if (!mCapturer) {
      return -1;
    }

    DesktopCapturer::SourceId sourceId = atoi(mDeviceUniqueId.c_str());
    mCapturer->SelectSource(sourceId);
  }
  return 0;
}

DesktopCaptureImpl::DesktopCaptureImpl(const int32_t aId, const char* aUniqueId,
                                       const CaptureDeviceType aType)
    : mModuleId(aId),
      mTrackingId(mozilla::TrackingId(CaptureEngineToTrackingSourceStr([&] {
                                        switch (aType) {
                                          case CaptureDeviceType::Screen:
                                            return CaptureEngine::ScreenEngine;
                                          case CaptureDeviceType::Window:
                                            return CaptureEngine::WinEngine;
                                          case CaptureDeviceType::Browser:
                                            return CaptureEngine::BrowserEngine;
                                          default:
                                            return CaptureEngine::InvalidEngine;
                                        }
                                      }()),
                                      aId)),
      mDeviceUniqueId(aUniqueId),
      mDeviceType(aType),
      mTimeEvent(EventWrapper::Create()),
      mLastFrameTimeMs(rtc::TimeMillis()),
      mRunning(false),
      mCallbacks("DesktopCaptureImpl::mCallbacks") {}

DesktopCaptureImpl::~DesktopCaptureImpl() {
  mTimeEvent->Set();
  if (mCaptureThread) {
#if defined(_WIN32)
    mCaptureThread->Stop();
#else
    mCaptureThread->Finalize();
#endif
  }
}

void DesktopCaptureImpl::RegisterCaptureDataCallback(
    rtc::VideoSinkInterface<VideoFrame>* aDataCallback) {
  auto callbacks = mCallbacks.Lock();
  callbacks->insert(aDataCallback);
}

void DesktopCaptureImpl::DeRegisterCaptureDataCallback(
    rtc::VideoSinkInterface<VideoFrame>* aDataCallback) {
  auto callbacks = mCallbacks.Lock();
  auto it = callbacks->find(aDataCallback);
  if (it != callbacks->end()) {
    callbacks->erase(it);
  }
}

int32_t DesktopCaptureImpl::StopCaptureIfAllClientsClose() {
  {
    auto callbacks = mCallbacks.Lock();
    if (!callbacks->empty()) {
      return 0;
    }
  }
  return StopCapture();
}

int32_t DesktopCaptureImpl::DeliverCapturedFrame(
    webrtc::VideoFrame& aCaptureFrame) {
  // Set the capture time
  aCaptureFrame.set_timestamp_us(rtc::TimeMicros());

  if (aCaptureFrame.render_time_ms() == mLastFrameTimeMs) {
    // We don't allow the same capture time for two frames, drop this one.
    return -1;
  }
  mLastFrameTimeMs = aCaptureFrame.render_time_ms();

  auto callbacks = mCallbacks.Lock();
  for (auto* callback : *callbacks) {
    callback->OnFrame(aCaptureFrame);
  }

  return 0;
}

// Originally copied from VideoCaptureImpl::IncomingFrame, but has diverged
// somewhat. See Bug 1038324 and bug 1738946.
int32_t DesktopCaptureImpl::IncomingFrame(
    uint8_t* aVideoFrame, size_t aVideoFrameLength, size_t aWidthWithPadding,
    const VideoCaptureCapability& aFrameInfo) {
  int64_t startProcessTime = rtc::TimeNanos();
  rtc::CritScope cs(&mApiCs);

  const int32_t width = aFrameInfo.width;
  const int32_t height = aFrameInfo.height;

  // Not encoded, convert to I420.
  if (aFrameInfo.videoType != VideoType::kMJPEG &&
      CalcBufferSize(aFrameInfo.videoType, width, abs(height)) !=
          aVideoFrameLength) {
    RTC_LOG(LS_ERROR) << "Wrong incoming frame length.";
    return -1;
  }

  int stride_y = width;
  int stride_uv = (width + 1) / 2;

  // Setting absolute height (in case it was negative).
  // In Windows, the image starts bottom left, instead of top left.
  // Setting a negative source height, inverts the image (within LibYuv).

  mozilla::PerformanceRecorder<mozilla::CopyVideoStage> rec(
      "DesktopCaptureImpl::ConvertToI420"_ns, mTrackingId, width, abs(height));
  // TODO(nisse): Use a pool?
  rtc::scoped_refptr<I420Buffer> buffer =
      I420Buffer::Create(width, abs(height), stride_y, stride_uv, stride_uv);

  const int conversionResult = libyuv::ConvertToI420(
      aVideoFrame, aVideoFrameLength, buffer->MutableDataY(), buffer->StrideY(),
      buffer->MutableDataU(), buffer->StrideU(), buffer->MutableDataV(),
      buffer->StrideV(), 0, 0,  // No Cropping
      static_cast<int>(aWidthWithPadding), height, width, height,
      libyuv::kRotate0, ConvertVideoType(aFrameInfo.videoType));
  if (conversionResult != 0) {
    RTC_LOG(LS_ERROR) << "Failed to convert capture frame from type "
                      << static_cast<int>(aFrameInfo.videoType) << "to I420.";
    return -1;
  }
  rec.Record();

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

int32_t DesktopCaptureImpl::SetCaptureRotation(VideoRotation aRotation) {
  MOZ_ASSERT_UNREACHABLE("Unused");
  return -1;
}

bool DesktopCaptureImpl::SetApplyRotation(bool aEnable) { return true; }

void DesktopCaptureImpl::LazyInitCaptureThread() {
  MOZ_ASSERT(mCapturer,
             "DesktopCapturer must be initialized before the capture thread");
  if (mCaptureThread) {
    return;
  }
#if defined(_WIN32)
  mCaptureThread = std::make_unique<rtc::PlatformUIThread>(
      std::function([self = (void*)this]() { Run(self); }),
      "ScreenCaptureThread", rtc::ThreadAttributes{});
  mCaptureThread->RequestCallbackTimer(mMaxFPSNeeded);
#else
  auto self = rtc::scoped_refptr<DesktopCaptureImpl>(this);
  mCaptureThread =
      std::make_unique<rtc::PlatformThread>(rtc::PlatformThread::SpawnJoinable(
          [self] { self->process(); }, "ScreenCaptureThread"));
#endif
  mRunning = true;
}

int32_t DesktopCaptureImpl::StartCapture(
    const VideoCaptureCapability& aCapability) {
  rtc::CritScope lock(&mApiCs);
  // See Bug 1780884 for followup on understanding why multiple calls happen.
  // MOZ_ASSERT(!mRunning, "Capture must be stopped before Start() can be
  // called");
  if (mRunning) {
    return 0;
  }

  if (int32_t err = LazyInitDesktopCapturer(); err) {
    return err;
  }
  mRunning = true;
  mRequestedCapability = aCapability;
  mMaxFPSNeeded = mRequestedCapability.maxFPS > 0
                      ? 1000 / mRequestedCapability.maxFPS
                      : 1000;
  LazyInitCaptureThread();

  return 0;
}

bool DesktopCaptureImpl::FocusOnSelectedSource() {
  if (uint32_t err = LazyInitDesktopCapturer(); err) {
    return false;
  }

  return mCapturer->FocusOnSelectedSource();
}

int32_t DesktopCaptureImpl::StopCapture() {
  if (mRunning) {
    mRunning = false;
    MOZ_ASSERT(mCaptureThread, "Capturer thread should be initialized.");

#if defined(_WIN32)
    // thread is guaranteed stopped before this returns
    mCaptureThread->Stop();
#else
    // thread is guaranteed stopped before this returns
    mCaptureThread->Finalize();
#endif
    mCapturer.reset();
    mCapturerStarted = false;
    mCaptureThread.reset();
    return 0;
  }
  return -1;
}

bool DesktopCaptureImpl::CaptureStarted() { return mRunning; }

int32_t DesktopCaptureImpl::CaptureSettings(VideoCaptureCapability& aSettings) {
  MOZ_ASSERT_UNREACHABLE("Unused");
  return -1;
}

void DesktopCaptureImpl::OnCaptureResult(DesktopCapturer::Result aResult,
                                         std::unique_ptr<DesktopFrame> aFrame) {
  if (!aFrame) {
    return;
  }
  uint8_t* videoFrame = aFrame->data();
  VideoCaptureCapability frameInfo;
  frameInfo.width = aFrame->size().width();
  frameInfo.height = aFrame->size().height();
  frameInfo.videoType = VideoType::kARGB;

  size_t videoFrameLength =
      frameInfo.width * frameInfo.height * DesktopFrame::kBytesPerPixel;
  IncomingFrame(videoFrame, videoFrameLength,
                aFrame->stride() / DesktopFrame::kBytesPerPixel, frameInfo);
}

void DesktopCaptureImpl::process() {
  // We need to call Start on the same thread we call CaptureFrame on.
  if (!mCapturerStarted) {
    mCapturer->Start(this);
    mCapturerStarted = true;
  }
#if defined(WEBRTC_WIN)
  ProcessIter();
#else
  do {
    ProcessIter();
  } while (mRunning);
#endif
}

void DesktopCaptureImpl::ProcessIter() {
// We should deliver at least one frame before stopping
#if !defined(_WIN32)
  int64_t startProcessTime = rtc::TimeNanos();
#endif

  // Don't leak while we're looping
  base::ScopedNSAutoreleasePool autoreleasepool;

#if defined(WEBRTC_MAC)
  // Give cycles to the RunLoop so frame callbacks can happen
  CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, true);
#endif

  mCapturer->CaptureFrame();

#if !defined(_WIN32)
  const int32_t processTimeMs = static_cast<int32_t>(
      (rtc::TimeNanos() - startProcessTime) / rtc::kNumNanosecsPerMillisec);
  // Use at most x% CPU or limit framerate
  const float sleepTimeFactor = (100.0f / kMaxDesktopCaptureCpuUsage) - 1.0f;
  const int32_t sleepTimeMs =
      static_cast<int32_t>(sleepTimeFactor * static_cast<float>(processTimeMs));
  mTimeEvent->Wait(std::max(static_cast<int32_t>(mMaxFPSNeeded), sleepTimeMs));
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
