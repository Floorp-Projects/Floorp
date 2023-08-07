/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoEngine.h"
#include "libwebrtcglue/SystemTime.h"
#include "video_engine/desktop_capture_impl.h"
#include "system_wrappers/include/clock.h"
#ifdef WEBRTC_ANDROID
#  include "modules/video_capture/video_capture.h"
#endif

#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/jni/Utils.h"
#endif

#if defined(ANDROID)
namespace webrtc {
int32_t SetCaptureAndroidVM(JavaVM* javaVM);
}
#endif

namespace mozilla::camera {

#undef LOG
#undef LOG_ENABLED
mozilla::LazyLogModule gVideoEngineLog("VideoEngine");
#define LOG(args) MOZ_LOG(gVideoEngineLog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() MOZ_LOG_TEST(gVideoEngineLog, mozilla::LogLevel::Debug)

#if defined(ANDROID)
int VideoEngine::SetAndroidObjects() {
  LOG(("%s", __PRETTY_FUNCTION__));

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

int32_t VideoEngine::CreateVideoCapture(const char* aDeviceUniqueIdUTF8,
                                        webrtc::VideoCaptureOptions* aOptions) {
  LOG(("%s", __PRETTY_FUNCTION__));
  MOZ_ASSERT(aDeviceUniqueIdUTF8);

  int32_t id = GenerateId();
  LOG(("CaptureDeviceInfo.type=%s id=%d", mCaptureDevInfo.TypeName(), id));

  for (auto& it : mCaps) {
    if (it.second.VideoCapture() &&
        it.second.VideoCapture()->CurrentDeviceName() &&
        strcmp(it.second.VideoCapture()->CurrentDeviceName(),
               aDeviceUniqueIdUTF8) == 0) {
      mIdMap.emplace(id, it.first);
      return id;
    }
  }

  CaptureEntry entry = {-1, nullptr};

  if (mCaptureDevInfo.type == CaptureDeviceType::Camera) {
    if (aOptions) {
      entry = CaptureEntry(id, webrtc::VideoCaptureFactory::Create(
                                   aOptions, aDeviceUniqueIdUTF8));
    } else {
      entry = CaptureEntry(
          id, webrtc::VideoCaptureFactory::Create(aDeviceUniqueIdUTF8));
    }
    if (entry.VideoCapture()) {
      entry.VideoCapture()->SetApplyRotation(true);
    }
  } else {
#ifndef WEBRTC_ANDROID
    entry = CaptureEntry(
        id, rtc::scoped_refptr<webrtc::VideoCaptureModule>(
                webrtc::DesktopCaptureImpl::Create(id, aDeviceUniqueIdUTF8,
                                                   mCaptureDevInfo.type)));
#else
    MOZ_ASSERT("CreateVideoCapture NO DESKTOP CAPTURE IMPL ON ANDROID" ==
               nullptr);
#endif
  }
  mCaps.emplace(id, std::move(entry));
  mIdMap.emplace(id, id);
  return id;
}

int VideoEngine::ReleaseVideoCapture(const int32_t aId) {
  bool found = false;

#ifdef DEBUG
  {
    auto it = mIdMap.find(aId);
    MOZ_ASSERT(it != mIdMap.end());
    Unused << it;
  }
#endif

  for (auto& it : mIdMap) {
    if (it.first != aId && it.second == mIdMap[aId]) {
      // There are other tracks still using this hardware.
      found = true;
    }
  }

  if (!found) {
    WithEntry(aId, [&found](CaptureEntry& cap) {
      cap.mVideoCaptureModule = nullptr;
      found = true;
    });
    MOZ_ASSERT(found);
    if (found) {
      auto it = mCaps.find(mIdMap[aId]);
      MOZ_ASSERT(it != mCaps.end());
      mCaps.erase(it);
    }
  }

  mIdMap.erase(aId);
  return found ? 0 : (-1);
}

std::shared_ptr<webrtc::VideoCaptureModule::DeviceInfo>
VideoEngine::GetOrCreateVideoCaptureDeviceInfo(
    webrtc::VideoCaptureOptions* aOptions) {
  LOG(("%s", __PRETTY_FUNCTION__));
  webrtc::Timestamp currentTime = webrtc::Timestamp::Micros(0);

  const char* capDevTypeName =
      CaptureDeviceInfo(mCaptureDevInfo.type).TypeName();

  if (mDeviceInfo) {
    LOG(("Device cache available."));
    // Camera cache is invalidated by HW change detection elsewhere
    if (mCaptureDevInfo.type == CaptureDeviceType::Camera) {
      LOG(("returning cached CaptureDeviceInfo of type %s", capDevTypeName));
      return mDeviceInfo;
    }
    // Screen sharing cache is invalidated after the expiration time
    currentTime = WebrtcSystemTime();
    LOG(("Checking expiry, fetched current time of: %" PRId64,
         currentTime.ms()));
    LOG(("device cache expiration is %" PRId64, mExpiryTime.ms()));
    if (currentTime <= mExpiryTime) {
      LOG(("returning cached CaptureDeviceInfo of type %s", capDevTypeName));
      return mDeviceInfo;
    }
  }

  if (currentTime.IsZero()) {
    currentTime = WebrtcSystemTime();
    LOG(("Fetched current time of: %" PRId64, currentTime.ms()));
  }
  mExpiryTime = currentTime + webrtc::TimeDelta::Millis(kCacheExpiryPeriodMs);
  LOG(("new device cache expiration is %" PRId64, mExpiryTime.ms()));
  LOG(("creating a new VideoCaptureDeviceInfo of type %s", capDevTypeName));

  switch (mCaptureDevInfo.type) {
    case CaptureDeviceType::Camera: {
#ifdef MOZ_WIDGET_ANDROID
      if (SetAndroidObjects()) {
        LOG(("VideoEngine::SetAndroidObjects Failed"));
        break;
      }
#endif
      if (aOptions) {
        mDeviceInfo.reset(
            webrtc::VideoCaptureFactory::CreateDeviceInfo(aOptions));
      } else {
        mDeviceInfo.reset(webrtc::VideoCaptureFactory::CreateDeviceInfo());
      }

      LOG(("CaptureDeviceType::Camera: Finished creating new device."));
      break;
    }
    // Window and Screen and Browser (tab) types are handled by DesktopCapture
    case CaptureDeviceType::Browser:
    case CaptureDeviceType::Window:
    case CaptureDeviceType::Screen: {
#if !defined(WEBRTC_ANDROID) && !defined(WEBRTC_IOS)
      mDeviceInfo = webrtc::DesktopCaptureImpl::CreateDeviceInfo(
          mId, mCaptureDevInfo.type);
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

already_AddRefed<VideoEngine> VideoEngine::Create(
    const CaptureDeviceType& aCaptureDeviceType) {
  LOG(("%s", __PRETTY_FUNCTION__));
  return do_AddRef(new VideoEngine(aCaptureDeviceType));
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
  static int sId = 0;
  return mId = sId++;
}

VideoEngine::VideoEngine(const CaptureDeviceType& aCaptureDeviceType)
    : mId(0), mCaptureDevInfo(aCaptureDeviceType), mDeviceInfo(nullptr) {
  LOG(("%s", __PRETTY_FUNCTION__));
  LOG(("Creating new VideoEngine with CaptureDeviceType %s",
       mCaptureDevInfo.TypeName()));
}

VideoEngine::~VideoEngine() {
  MOZ_ASSERT(mCaps.empty());
  MOZ_ASSERT(mIdMap.empty());
}

}  // namespace mozilla::camera
