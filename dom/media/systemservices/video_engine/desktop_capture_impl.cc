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

#include <cstdlib>
#include <memory>
#include <string>

#include "CamerasTypes.h"
#include "VideoEngine.h"
#include "VideoUtils.h"
#include "api/video/i420_buffer.h"
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
#include "modules/desktop_capture/desktop_and_cursor_composer.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer_differ_wrapper.h"
#include "modules/video_capture/video_capture.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/TimeStamp.h"
#include "nsThreadUtils.h"
#include "tab_capturer.h"

using mozilla::NewRunnableMethod;
using mozilla::TabCapturerWebrtc;
using mozilla::TimeDuration;
using mozilla::camera::CaptureDeviceType;
using mozilla::camera::CaptureEngine;

static void CaptureFrameOnThread(nsITimer* aTimer, void* aClosure) {
  static_cast<webrtc::DesktopCaptureImpl*>(aClosure)->CaptureFrameOnThread();
}

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
  }
  options.set_allow_cropping_window_capturer(true);
#  if defined(RTC_ENABLE_WIN_WGC)
  if (mozilla::StaticPrefs::media_webrtc_capture_screen_allow_wgc()) {
    options.set_allow_wgc_screen_capturer(true);
    options.set_allow_wgc_zero_hertz(
        mozilla::StaticPrefs::media_webrtc_capture_wgc_allow_zero_hertz());
  }
  if (mozilla::StaticPrefs::media_webrtc_capture_window_allow_wgc()) {
    options.set_allow_wgc_window_capturer(true);
    options.set_allow_wgc_zero_hertz(
        mozilla::StaticPrefs::media_webrtc_capture_wgc_allow_zero_hertz());
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

static std::unique_ptr<DesktopCapturer> CreateTabCapturer(
    const DesktopCaptureOptions& options, DesktopCapturer::SourceId aSourceId,
    nsCOMPtr<nsISerialEventTarget> aCaptureThread) {
  std::unique_ptr<DesktopCapturer> capturer =
      TabCapturerWebrtc::Create(aSourceId, std::move(aCaptureThread));
  if (capturer && options.detect_updated_region()) {
    capturer.reset(new DesktopCapturerDifferWrapper(std::move(capturer)));
  }

  return capturer;
}

static bool UsePipewire() {
#if defined(WEBRTC_USE_PIPEWIRE)
  return mozilla::StaticPrefs::media_webrtc_capture_allow_pipewire() &&
         webrtc::DesktopCapturer::IsRunningUnderWayland();
#else
  return false;
#endif
}

static std::unique_ptr<DesktopCapturer> CreateDesktopCapturerAndThread(
    CaptureDeviceType aDeviceType, DesktopCapturer::SourceId aSourceId,
    nsIThread** aOutThread) {
  DesktopCaptureOptions options = CreateDesktopCaptureOptions();
  std::unique_ptr<DesktopCapturer> capturer;

  auto ensureThread = [&]() {
    if (*aOutThread) {
      return *aOutThread;
    }

    nsIThreadManager::ThreadCreationOptions threadOptions;
#if defined(XP_WIN) || defined(XP_MACOSX)
    // Windows desktop capture needs a UI thread.
    // Mac screen capture needs a thread with a CFRunLoop.
    threadOptions.isUiThread = true;
#endif
    NS_NewNamedThread("DesktopCapture", aOutThread, nullptr, threadOptions);
    return *aOutThread;
  };

  if ((aDeviceType == CaptureDeviceType::Screen ||
       aDeviceType == CaptureDeviceType::Window) &&
      UsePipewire()) {
    capturer = DesktopCapturer::CreateGenericCapturer(options);
    if (!capturer) {
      return capturer;
    }

    capturer = std::make_unique<DesktopAndCursorComposer>(std::move(capturer),
                                                          options);
  } else if (aDeviceType == CaptureDeviceType::Screen) {
    capturer = DesktopCapturer::CreateScreenCapturer(options);
    if (!capturer) {
      return capturer;
    }

    capturer->SelectSource(aSourceId);

    capturer = std::make_unique<DesktopAndCursorComposer>(std::move(capturer),
                                                          options);
  } else if (aDeviceType == CaptureDeviceType::Window) {
#if defined(RTC_ENABLE_WIN_WGC)
    options.set_allow_wgc_capturer_fallback(true);
#endif
    capturer = DesktopCapturer::CreateWindowCapturer(options);
    if (!capturer) {
      return capturer;
    }

    capturer->SelectSource(aSourceId);

    capturer = std::make_unique<DesktopAndCursorComposer>(std::move(capturer),
                                                          options);
  } else if (aDeviceType == CaptureDeviceType::Browser) {
    // XXX We don't capture cursors, so avoid the extra indirection layer. We
    // could also pass null for the pMouseCursorMonitor.
    capturer = CreateTabCapturer(options, aSourceId, ensureThread());
  } else {
    MOZ_ASSERT(!capturer);
    return capturer;
  }

  MOZ_ASSERT(capturer);
  ensureThread();

  return capturer;
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
      mControlThread(mozilla::GetCurrentSerialEventTarget()),
      mNextFrameMinimumTime(Timestamp::Zero()),
      mCallbacks("DesktopCaptureImpl::mCallbacks") {}

DesktopCaptureImpl::~DesktopCaptureImpl() {
  MOZ_ASSERT(!mCaptureThread);
  MOZ_ASSERT(!mRequestedCapability);
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

int32_t DesktopCaptureImpl::SetCaptureRotation(VideoRotation aRotation) {
  MOZ_ASSERT_UNREACHABLE("Unused");
  return -1;
}

bool DesktopCaptureImpl::SetApplyRotation(bool aEnable) { return true; }

int32_t DesktopCaptureImpl::StartCapture(
    const VideoCaptureCapability& aCapability) {
  RTC_DCHECK_RUN_ON(&mControlThreadChecker);

  if (mRequestedCapability) {
    // Already initialized
    MOZ_ASSERT(*mRequestedCapability == aCapability);

    return 0;
  }

  MOZ_ASSERT(!mCaptureThread);

  DesktopCapturer::SourceId sourceId = std::stoi(mDeviceUniqueId);
  std::unique_ptr capturer = CreateDesktopCapturerAndThread(
      mDeviceType, sourceId, getter_AddRefs(mCaptureThread));

  MOZ_ASSERT(!capturer == !mCaptureThread);
  if (!capturer) {
    return -1;
  }

  mRequestedCapability = mozilla::Some(aCapability);
  mCaptureThreadChecker.Detach();

  MOZ_ALWAYS_SUCCEEDS(mCaptureThread->Dispatch(NS_NewRunnableFunction(
      "DesktopCaptureImpl::InitOnThread",
      [this, self = RefPtr(this), capturer = std::move(capturer),
       maxFps = std::max(aCapability.maxFPS, 1)]() mutable {
        InitOnThread(std::move(capturer), maxFps);
      })));

  return 0;
}

bool DesktopCaptureImpl::FocusOnSelectedSource() {
  RTC_DCHECK_RUN_ON(&mControlThreadChecker);
  if (!mCaptureThread) {
    MOZ_ASSERT_UNREACHABLE(
        "FocusOnSelectedSource must be called after StartCapture");
    return false;
  }

  bool success = false;
  MOZ_ALWAYS_SUCCEEDS(mozilla::SyncRunnable::DispatchToThread(
      mCaptureThread, NS_NewRunnableFunction(__func__, [&] {
        RTC_DCHECK_RUN_ON(&mCaptureThreadChecker);
        MOZ_ASSERT(mCapturer);
        success = mCapturer && mCapturer->FocusOnSelectedSource();
      })));
  return success;
}

int32_t DesktopCaptureImpl::StopCapture() {
  RTC_DCHECK_RUN_ON(&mControlThreadChecker);
  if (mRequestedCapability) {
    // Sync-cancel the capture timer so no CaptureFrame calls will come in after
    // we return.
    MOZ_ALWAYS_SUCCEEDS(mozilla::SyncRunnable::DispatchToThread(
        mCaptureThread,
        NewRunnableMethod(__func__, this,
                          &DesktopCaptureImpl::ShutdownOnThread)));

    mRequestedCapability = mozilla::Nothing();
  }

  if (mCaptureThread) {
    // CaptureThread shutdown.
    mCaptureThread->AsyncShutdown();
    mCaptureThread = nullptr;
  }

  return 0;
}

bool DesktopCaptureImpl::CaptureStarted() {
  MOZ_ASSERT_UNREACHABLE("Unused");
  return true;
}

int32_t DesktopCaptureImpl::CaptureSettings(VideoCaptureCapability& aSettings) {
  MOZ_ASSERT_UNREACHABLE("Unused");
  return -1;
}

void DesktopCaptureImpl::OnCaptureResult(DesktopCapturer::Result aResult,
                                         std::unique_ptr<DesktopFrame> aFrame) {
  RTC_DCHECK_RUN_ON(&mCaptureThreadChecker);
  if (!aFrame) {
    return;
  }

  const auto startProcessTime = Timestamp::Micros(rtc::TimeMicros());
  auto frameTime = startProcessTime;
  if (auto diff = startProcessTime - mNextFrameMinimumTime;
      diff < TimeDelta::Zero()) {
    if (diff > TimeDelta::Millis(-1)) {
      // Two consecutive frames within a millisecond is OK. It could happen due
      // to timing.
      frameTime = mNextFrameMinimumTime;
    } else {
      // Three consecutive frames within two milliseconds seems too much, drop
      // one.
      MOZ_ASSERT(diff >= TimeDelta::Millis(-2));
      RTC_LOG(LS_WARNING) << "DesktopCapture render time is getting too far "
                             "ahead. Framerate is unexpectedly high.";
      return;
    }
  }

  uint8_t* videoFrame = aFrame->data();
  VideoCaptureCapability frameInfo;
  frameInfo.width = aFrame->size().width();
  frameInfo.height = aFrame->size().height();
  frameInfo.videoType = VideoType::kARGB;

  size_t videoFrameLength =
      frameInfo.width * frameInfo.height * DesktopFrame::kBytesPerPixel;

  const int32_t width = frameInfo.width;
  const int32_t height = frameInfo.height;

  // Not encoded, convert to I420.
  if (frameInfo.videoType != VideoType::kMJPEG &&
      CalcBufferSize(frameInfo.videoType, width, abs(height)) !=
          videoFrameLength) {
    RTC_LOG(LS_ERROR) << "Wrong incoming frame length.";
    return;
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
      videoFrame, videoFrameLength, buffer->MutableDataY(), buffer->StrideY(),
      buffer->MutableDataU(), buffer->StrideU(), buffer->MutableDataV(),
      buffer->StrideV(), 0, 0,  // No Cropping
      aFrame->stride() / DesktopFrame::kBytesPerPixel, height, width, height,
      libyuv::kRotate0, ConvertVideoType(frameInfo.videoType));
  if (conversionResult != 0) {
    RTC_LOG(LS_ERROR) << "Failed to convert capture frame from type "
                      << static_cast<int>(frameInfo.videoType) << "to I420.";
    return;
  }
  rec.Record();

  NotifyOnFrame(VideoFrame::Builder()
                    .set_video_frame_buffer(buffer)
                    .set_timestamp_us(frameTime.us())
                    .build());

  const TimeDelta processTime =
      Timestamp::Micros(rtc::TimeMicros()) - startProcessTime;

  if (processTime > TimeDelta::Millis(10)) {
    RTC_LOG(LS_WARNING)
        << "Too long processing time of incoming frame with dimensions "
        << width << "x" << height << ": " << processTime.ms() << " ms";
  }
}

void DesktopCaptureImpl::NotifyOnFrame(const VideoFrame& aFrame) {
  RTC_DCHECK_RUN_ON(&mCaptureThreadChecker);
  MOZ_ASSERT(Timestamp::Millis(aFrame.render_time_ms()) >
             mNextFrameMinimumTime);
  // Set the next frame's minimum time to ensure two consecutive frames don't
  // have an identical render time (which is in milliseconds).
  mNextFrameMinimumTime =
      Timestamp::Millis(aFrame.render_time_ms()) + TimeDelta::Millis(1);
  auto callbacks = mCallbacks.Lock();
  for (auto* cb : *callbacks) {
    cb->OnFrame(aFrame);
  }
}

void DesktopCaptureImpl::InitOnThread(
    std::unique_ptr<DesktopCapturer> aCapturer, int aFramerate) {
  RTC_DCHECK_RUN_ON(&mCaptureThreadChecker);

  mCapturer = std::move(aCapturer);

  // We need to call Start on the same thread we call CaptureFrame on.
  mCapturer->Start(this);

  mCaptureTimer = NS_NewTimer();
  mRequestedCaptureInterval = mozilla::Some(
      TimeDuration::FromSeconds(1. / static_cast<double>(aFramerate)));

  CaptureFrameOnThread();
}

void DesktopCaptureImpl::ShutdownOnThread() {
  RTC_DCHECK_RUN_ON(&mCaptureThreadChecker);
  if (mCaptureTimer) {
    mCaptureTimer->Cancel();
    mCaptureTimer = nullptr;
  }

  // DesktopCapturer dtor blocks until fully shut down. TabCapturerWebrtc needs
  // the capture thread to be alive.
  mCapturer = nullptr;

  mRequestedCaptureInterval = mozilla::Nothing();
}

void DesktopCaptureImpl::CaptureFrameOnThread() {
  RTC_DCHECK_RUN_ON(&mCaptureThreadChecker);

  auto start = mozilla::TimeStamp::Now();
  mCapturer->CaptureFrame();
  auto end = mozilla::TimeStamp::Now();

  // Calculate next capture time.
  const auto duration = end - start;
  const auto timeUntilRequestedCapture = *mRequestedCaptureInterval - duration;

  // Use at most x% CPU or limit framerate
  constexpr float sleepTimeFactor =
      (100.0f / kMaxDesktopCaptureCpuUsage) - 1.0f;
  static_assert(sleepTimeFactor >= 0.0);
  static_assert(sleepTimeFactor < 100.0);
  const auto sleepTime = duration.MultDouble(sleepTimeFactor);

  mCaptureTimer->InitHighResolutionWithNamedFuncCallback(
      &::CaptureFrameOnThread, this,
      std::max(timeUntilRequestedCapture, sleepTime), nsITimer::TYPE_ONE_SHOT,
      "DesktopCaptureImpl::mCaptureTimer");
}

}  // namespace webrtc
