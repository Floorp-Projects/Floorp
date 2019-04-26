/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoEngine.h"
#include "video_engine/browser_capture_impl.h"
#include "video_engine/desktop_capture_impl.h"
#include "webrtc/system_wrappers/include/clock.h"
#ifdef WEBRTC_ANDROID
#  include "webrtc/modules/video_capture/video_capture.h"
#endif

#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/jni/Utils.h"
#endif

namespace mozilla {
namespace camera {

#undef LOG
#undef LOG_ENABLED
mozilla::LazyLogModule gVideoEngineLog("VideoEngine");
#define LOG(args) MOZ_LOG(gVideoEngineLog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() MOZ_LOG_TEST(gVideoEngineLog, mozilla::LogLevel::Debug)

int VideoEngine::sId = 0;
#if defined(ANDROID)
int VideoEngine::SetAndroidObjects() {
  LOG((__PRETTY_FUNCTION__));

  JavaVM* const javaVM = mozilla::jni::GetVM();
  if (!javaVM || webrtc::SetCaptureAndroidVM(javaVM) != 0) {
    LOG(("Could not set capture Android VM"));
    return -1;
  }
#  ifdef WEBRTC_INCLUDE_INTERNAL_VIDEO_RENDER
  if (webrtc::SetRenderAndroidVM(javaVM) != 0) {
    LOG(("Could not set render Android VM"));
    return -1;
  }
#  endif
  return 0;
}
#endif

void VideoEngine::CreateVideoCapture(int32_t& id,
                                     const char* deviceUniqueIdUTF8) {
  LOG((__PRETTY_FUNCTION__));
  MOZ_ASSERT(deviceUniqueIdUTF8);

  id = GenerateId();
  LOG(("CaptureDeviceInfo.type=%s id=%d", mCaptureDevInfo.TypeName(), id));

  for (auto& it : mCaps) {
    if (it.second.VideoCapture() &&
        it.second.VideoCapture()->CurrentDeviceName() &&
        strcmp(it.second.VideoCapture()->CurrentDeviceName(),
               deviceUniqueIdUTF8) == 0) {
      mIdMap.emplace(id, it.first);
      return;
    }
  }

  CaptureEntry entry = {-1, nullptr};

  if (mCaptureDevInfo.type == webrtc::CaptureDeviceType::Camera) {
    entry = CaptureEntry(
        id, webrtc::VideoCaptureFactory::Create(deviceUniqueIdUTF8));
    if (entry.VideoCapture()) {
      entry.VideoCapture()->SetApplyRotation(true);
    }
  } else {
#ifndef WEBRTC_ANDROID
#  ifdef MOZ_X11
    webrtc::VideoCaptureModule* captureModule;
    auto type = mCaptureDevInfo.type;
    nsresult result = NS_DispatchToMainThread(
        media::NewRunnableFrom([&captureModule, id, deviceUniqueIdUTF8,
                                type]() -> nsresult {
          captureModule =
              webrtc::DesktopCaptureImpl::Create(id, deviceUniqueIdUTF8, type);
          return NS_OK;
        }),
        nsIEventTarget::DISPATCH_SYNC);

    if (result == NS_OK) {
      entry = CaptureEntry(id, captureModule);
    } else {
      return;
    }
#  else
    entry = CaptureEntry(id, webrtc::DesktopCaptureImpl::Create(
                                 id, deviceUniqueIdUTF8, mCaptureDevInfo.type));
#  endif
#else
    MOZ_ASSERT("CreateVideoCapture NO DESKTOP CAPTURE IMPL ON ANDROID" ==
               nullptr);
#endif
  }
  mCaps.emplace(id, std::move(entry));
  mIdMap.emplace(id, id);
}

int VideoEngine::ReleaseVideoCapture(const int32_t id) {
  bool found = false;

#ifdef DEBUG
  {
    auto it = mIdMap.find(id);
    MOZ_ASSERT(it != mIdMap.end());
    Unused << it;
  }
#endif

  for (auto& it : mIdMap) {
    if (it.first != id && it.second == mIdMap[id]) {
      // There are other tracks still using this hardware.
      found = true;
    }
  }

  if (!found) {
    WithEntry(id, [&found](CaptureEntry& cap) {
      cap.mVideoCaptureModule = nullptr;
      found = true;
    });
    MOZ_ASSERT(found);
    if (found) {
      auto it = mCaps.find(mIdMap[id]);
      MOZ_ASSERT(it != mCaps.end());
      mCaps.erase(it);
    }
  }

  mIdMap.erase(id);
  return found ? 0 : (-1);
}

std::shared_ptr<webrtc::VideoCaptureModule::DeviceInfo>
VideoEngine::GetOrCreateVideoCaptureDeviceInfo() {
  LOG((__PRETTY_FUNCTION__));
  int64_t currentTime = 0;

  const char* capDevTypeName =
      webrtc::CaptureDeviceInfo(mCaptureDevInfo.type).TypeName();

  if (mDeviceInfo) {
    LOG(("Device cache available."));
    // Camera cache is invalidated by HW change detection elsewhere
    if (mCaptureDevInfo.type == webrtc::CaptureDeviceType::Camera) {
      LOG(("returning cached CaptureDeviceInfo of type %s", capDevTypeName));
      return mDeviceInfo;
    }
    // Screen sharing cache is invalidated after the expiration time
    currentTime = webrtc::Clock::GetRealTimeClock()->TimeInMilliseconds();
    LOG(("Checking expiry, fetched current time of: %" PRId64, currentTime));
    LOG(("device cache expiration is %" PRId64, mExpiryTimeInMs));
    if (currentTime <= mExpiryTimeInMs) {
      LOG(("returning cached CaptureDeviceInfo of type %s", capDevTypeName));
      return mDeviceInfo;
    }
  }

  if (currentTime == 0) {
    currentTime = webrtc::Clock::GetRealTimeClock()->TimeInMilliseconds();
    LOG(("Fetched current time of: %" PRId64, currentTime));
  }
  mExpiryTimeInMs = currentTime + kCacheExpiryPeriodMs;
  LOG(("new device cache expiration is %" PRId64, mExpiryTimeInMs));
  LOG(("creating a new VideoCaptureDeviceInfo of type %s", capDevTypeName));

  switch (mCaptureDevInfo.type) {
    case webrtc::CaptureDeviceType::Camera: {
#ifdef MOZ_WIDGET_ANDROID
      if (SetAndroidObjects()) {
        LOG(("VideoEngine::SetAndroidObjects Failed"));
        break;
      }
#endif
      mDeviceInfo.reset(webrtc::VideoCaptureFactory::CreateDeviceInfo());
      LOG(("webrtc::CaptureDeviceType::Camera: Finished creating new device."));
      break;
    }
    case webrtc::CaptureDeviceType::Browser: {
      mDeviceInfo.reset(webrtc::BrowserDeviceInfoImpl::CreateDeviceInfo());
      LOG((
          "webrtc::CaptureDeviceType::Browser: Finished creating new device."));
      break;
    }
    // Window and Screen types are handled by DesktopCapture
    case webrtc::CaptureDeviceType::Window:
    case webrtc::CaptureDeviceType::Screen: {
#if !defined(WEBRTC_ANDROID) && !defined(WEBRTC_IOS)
      mDeviceInfo.reset(webrtc::DesktopCaptureImpl::CreateDeviceInfo(
          mId, mCaptureDevInfo.type));
      LOG(("screen capture: Finished creating new device."));
#else
      MOZ_ASSERT(
          "GetVideoCaptureDeviceInfo NO DESKTOP CAPTURE IMPL ON ANDROID" ==
          nullptr);
      mDeviceInfo.reset();
#endif
      break;
    }
  }
  LOG(("EXIT %s", __PRETTY_FUNCTION__));
  return mDeviceInfo;
}

const UniquePtr<const webrtc::Config>& VideoEngine::GetConfiguration() {
  return mConfig;
}

already_AddRefed<VideoEngine> VideoEngine::Create(
    UniquePtr<const webrtc::Config>&& aConfig) {
  LOG((__PRETTY_FUNCTION__));
  LOG(("Creating new VideoEngine with CaptureDeviceType %s",
       aConfig->Get<webrtc::CaptureDeviceInfo>().TypeName()));
  return do_AddRef(new VideoEngine(std::move(aConfig)));
}

VideoEngine::CaptureEntry::CaptureEntry(
    int32_t aCapnum, rtc::scoped_refptr<webrtc::VideoCaptureModule> aCapture)
    : mCapnum(aCapnum), mVideoCaptureModule(aCapture) {}

rtc::scoped_refptr<webrtc::VideoCaptureModule>
VideoEngine::CaptureEntry::VideoCapture() {
  return mVideoCaptureModule;
}

int32_t VideoEngine::CaptureEntry::Capnum() const { return mCapnum; }

bool VideoEngine::WithEntry(
    const int32_t entryCapnum,
    const std::function<void(CaptureEntry& entry)>&& fn) {
#ifdef DEBUG
  {
    auto it = mIdMap.find(entryCapnum);
    MOZ_ASSERT(it != mIdMap.end());
    Unused << it;
  }
#endif

  auto it = mCaps.find(mIdMap[entryCapnum]);
  MOZ_ASSERT(it != mCaps.end());
  if (it == mCaps.end()) {
    return false;
  }
  fn(it->second);
  return true;
}

int32_t VideoEngine::GenerateId() {
  // XXX Something better than this (a map perhaps, or a simple boolean TArray,
  // given the number in-use is O(1) normally!)
  return mId = sId++;
}

VideoEngine::VideoEngine(UniquePtr<const webrtc::Config>&& aConfig)
    : mId(0),
      mCaptureDevInfo(aConfig->Get<webrtc::CaptureDeviceInfo>()),
      mDeviceInfo(nullptr),
      mConfig(std::move(aConfig)) {
  LOG((__PRETTY_FUNCTION__));
}

}  // namespace camera
}  // namespace mozilla
